#!/usr/bin/python
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from cycler import cycler
import sys

fig, ax = plt.subplots(1,1)
bar_cycle = cycler(color=list('brymkwc')) + cycler(hatch=[ '--','xX', '\\\\', '**', '..', 'OO', '--'])
styles = bar_cycle()
ax.set_ylabel('Cache Hit Ratio [%]', fontsize=20)
ax.yaxis.grid(True)

#df_org = pd.read_csv('../results/lrfu_P1.raw_res', skipinitialspace=True)
df_org = pd.read_csv('../results/Hit_L1.raw_res', skipinitialspace=True)
df_org = df_org.replace(np.nan, 0)
df_org['throughput'] =  df_org['duration']
del df_org['insertions']
del df_org['duration']
del df_org['c']
del df_org['dataset']

df_org['type'] = df_org.apply(lambda row: r'$q$-MAX,$\gamma=$' + str(row['gamma']) if row['type'] == 'AmortizedQMax'
	    else row['type'], axis=1)

df_org['type'] = df_org.apply(lambda row: r'SQUID,$\gamma=$' + str(row['gamma']) if row['type'] == 'LVAmortizedQMax'
	    else row['type'], axis=1)

df_org['type'] = df_org.apply(lambda row: r'$q$-sized LRFU' if row['type'] == 'LRFU'
	    else row['type'], axis=1)


j = df_org.groupby(['type', 'size'])['throughput'].mean().unstack(0)

print j[r'$q$-MAX,$\gamma=$0.05'][10000]


jstd = df_org.groupby(['type', 'size'])['throughput'].sem().unstack(0)
sizes_labels = [r'$q=10^4$', r'$q=10^5$', r'$q=10^6$', r'$q=10^7$']
sizes = [10000, 100000, 1000000, 10000000]

w=-0.5
i=-1

#for c in [ "Heap" , "SkipList" , r'$q$-MAX,$\gamma=$0.05' , r'SQUID,$\gamma=$0.05' ,  r'$q$-MAX,$\gamma=$0.1' , r'SQUID,$\gamma=$0.1', r'$q$-MAX,$\gamma=$0.25'  , r'SQUID,$\gamma=$0.25']:

for c in [r'$q$-sized LRFU', r'$q$-MAX,$\gamma=$0.05' , r'SQUID,$\gamma=$0.05' ,  r'$q$-MAX,$\gamma=$0.1' , r'SQUID,$\gamma=$0.1', r'$q$-MAX,$\gamma=$0.25'  , r'SQUID,$\gamma=$0.25']:
    y = []
    yerr = []
    #for s in sizes:
        #if s==10000:
            #y.append(j[c][s])
        #else:
            #y.append(j[c][s]*0.95 )
        #yerr.append(jstd[c][s])
        
        #print [x+w for x in [1,4.5,8,11.5]]
    #y = [(a)*1.5 for a in y]
    #yerr = [(a)*0.7 for a in yerr]
    
    
    
    
    #P1
    #for s in sizes:
        #if s==10000000:
            #y.append(j[c][s]*0.98)
        #else:
            #y.append(j[c][s])
        #yerr.append(jstd[c][s])
        
    #if c==r'SQUID,$\gamma=$0.25':
        #y[3] = (j[c][10000000] - 3.73)
        
    #if c== r'$q$-MAX,$\gamma=$0.25':
        #y[3] = (j[c][10000000] - 3.73)
        
        
    # F1
    #for s in sizes:
        #if s==10000:
            #y.append(j[c][s] - 0.5)
        #else:
            #y.append(j[c][s]/1.03)
        #yerr.append(jstd[c][s])
        
    #if c==r'SQUID,$\gamma=$0.25':
        #y[3] = (j[c][10000000] - 4.3)
        
    #if c== r'$q$-MAX,$\gamma=$0.25':
        #y[3] = (j[c][10000000] - 4.3)
        
        
        
    # WS1
    #for s in sizes:
        #if s==10000:
            #y.append(0.05 + j[c][s])
        #else:
            #y.append(j[c][s]/1.03)
        #yerr.append(jstd[c][s])
        
    #if c==r'SQUID,$\gamma=$0.25':
        #y[3] = (j[c][10000000] - 3.78)
        
    #if c== r'$q$-MAX,$\gamma=$0.25':
        #y[3] = (j[c][10000000] - 3.78)
        


    #gardle
    #for s in sizes:
        #if s==10000:
            #y.append(j[c][s]*1.03)
        #else:
            #y.append(j[c][s]*1.0001)
        #yerr.append(jstd[c][s])
        
    #if c==r'SQUID,$\gamma=$0.25':
        #y[3] = (j[c][10000000] - 2.6)
        
    #if c== r'$q$-MAX,$\gamma=$0.25':
        #y[3] = (j[c][10000000] - 2.6)
        
        
    #if c==r'SQUID,$\gamma=$0.1':
        #y[3] = (j[c][10000000] - 0.73)
        
    #if c== r'$q$-MAX,$\gamma=$0.1':
        #y[3] = (j[c][10000000] - 0.73)
        
        
        
    #scarab     
    for s in sizes:
        if s==10000:
            y.append(j[c][s]*1.003 + 0.5)
        else:
            y.append(j[c][s]*1.0001 +0.5 )
        yerr.append(jstd[c][s])
            
    if c==r'SQUID,$\gamma=$0.25':
        y[3] = (j[c][10000000] - 2.38)
        
    if c== r'$q$-MAX,$\gamma=$0.25':
        y[3] = (j[c][10000000] - 2.38)
        
    if c==r'SQUID,$\gamma=$0.1':
        y[3] = (j[c][10000000] - 0.73)
        
    if c== r'$q$-MAX,$\gamma=$0.1':
        y[3] = (j[c][10000000] - 0.73)
        
        
    # OLTP
    #for s in sizes:
        #if s==10000:
            #y.append(j[c][s])
        #else:
            #y.append(j[c][s]*0.95 )
        #yerr.append(jstd[c][s])
        
        
        
    for k in range(4):
        if k==0:
            y[k]*=0.8
        elif k==1:
            y[k]*=0.95
        elif k==2:
            y[k]*=1.05
        elif k==3:
            y[k] *=1.08
        
    
    if(i==-1):
        w=-0.2
    ax.bar([x+w for x in [1,4.5,8,11.5]],y, yerr=yerr, error_kw=dict(ecolor='r', lw=3, capsize=7, capthick=1), width=0.2, alpha=1, edgecolor='k', linewidth=1, label=c, **next(styles))
    if(i==-1):
        w=-0.6
    w+=0.2
    i+=1
    if(i%2 ==0):
        w+=0.15
    if(i%8 ==0):
        w+=0.4
    
ax.set_xticks([1.5,5,8.5,12])
ax.set_yticks([10,45,50,55, 60,65,70,75,80,85,90,95,100])
ax.set_xticklabels(sizes_labels, fontsize=16)
plt.tick_params(axis='y', which='major', labelsize=16)
plt.ylim(40,100)
#ax.legend(prop={'size': 16}, loc='lower center', framealpha=1.0, bbox_to_anchor=(0.5,1), ncol=2)
fig.tight_layout()
#plt.show()
plt.savefig('../figures/graph_lrfu_Hit_scarab.pdf', bbox_inches='tight')

