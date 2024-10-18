#!/usr/bin/python
import matplotlib as mpl
rc_fonts = {
    "font.family": "sans-serif",
    'font.serif': 'Linux Libertine O'
}
mpl.rcParams.update(rc_fonts)



import numpy as np
import matplotlib.pyplot as plt
from cycler import cycler
import sys


def load_squid(file_name, threshold):
    fin = open(file_name, "r")
    hit_rate = 0
    while True:
        try:
            content = fin.readline()
            idx = content.find("Counter = ") + len("Counter = ")
            idy = content.find(". Hit")
            counter = int(content[idx: idy])

            idx = content.find(". Hit = ") + len(". Hit = ")
            idy = content.find(". Threshold")
            hit = int(content[idx: idy])
            if (counter > threshold):
                break
            else:
                hit_rate = float(hit) / counter
        except:
            break

    return hit_rate

def load_lrfu(file_name, threshold):
    fin = open(file_name, "r")
    hit_rate = 0
    i = 0
    while True:
        try:
            contents = fin.readline().split("\n")[0].split("/")
            counter = int(contents[1])
            hit = int(contents[0])

            if (counter > threshold):
                break
            else:
                hit_rate = float(hit) / counter
        except:
            break
    
    return hit_rate

def load_qmax(file_name, threshold):
    
    fin = open(file_name, "r")
    hit_rates = []
    while True:
        try:
            content = fin.readline().split("\n")[0]
            hit_rates.append(float(content))
        except:
            break

    return hit_rates

fig = plt.figure(figsize=(29, 9))

bar_cycle = cycler(color=['cyan', 'red', 'blueviolet', 'yellow', 'lime', 'black', 'hotpink']) + cycler(hatch=[ '--','xX', '\\\\', '**', '..', 'OO', '--'])
styles_gen = bar_cycle()

styles = []
#sizes_labels = [r'$q=\frac{2^{14}}{3}$', r'$q=\frac{2^{16}}{3}$', r'$q=\frac{2^{17}}{3}$', r'$q=\frac{2^{18}}{3}$', r'$q=\frac{2^{19}}{3}$', r'$q=2^{18}$']
# sizes_labels = [r'$2^{14}$', r'  $2^{16}$', r'$2^{17}$', r'$2^{18}$', r'$2^{19}$', r'$3 \cdot 2^{18}$']
sizes_labels = [r'$2^{16}$', r'$2^{17}$', r'$2^{18}$', r'$2^{19}$', r'$3 \cdot 2^{18}$', r"$3 \cdot 2^{19}$"]

for i in range(len(sizes_labels)):
    styles.append(next(styles_gen))



w=-0.5
i=-1

#for c in [ "Heap" , "SkipList" , r'$q$-MAX,$\gamma=$0.05' , r'SQUID,$\gamma=$0.05' ,  r'$q$-MAX,$\gamma=$0.1' , r'SQUID,$\gamma=$0.1', r'$q$-MAX,$\gamma=$0.25'  , r'SQUID,$\gamma=$0.25']:

exp_names = [r"Vanilla LRFU", "Vanilla LRU", r"CPU SQUID + LRFU", r"P4 (approximated) SQUID + LRFU", r"P4-LRU", "PPD"]

file_name_middle = ["lrfu", "lru", "hashing_mc_lrfu", "hashing_mc_lrfu_approx", "p4-lru", "aifo"]

# sizes = [[4096, 4], [16384, 4], [32768, 4], [65536, 4], [65536, 8], [65536, 12]]
sizes = [[16384, 4], [32768, 4], [65536, 4], [65536, 8], [65536, 12], [131072, 12]]


x_coord = [1, 4.5, 8, 11.5, 15, 18.5]

nfig = -1
for dataset in ["S2"]:
    nfig += 1
    ax = fig.add_subplot(1, 2, nfig + 1)

    
    i = -1
    w = -0.3
    ax.set_ylabel('Cache Hit Ratio [%]', fontsize=40)
    ax.set_xlabel(r'Cache size', fontsize=40)
    # ax.set_xlabel("$q = $", fontsize=30)
    ax.yaxis.grid(True)

    for c in exp_names:
        y = []
        yerr = []

        i += 1

        if c[0] == 'V' and c[-2] == "F":
            for j, size in enumerate(sizes):
                file_name = "results_S2/log_" + file_name_middle[i] + "_{}_{}_{}.txt".format(dataset, size[0], size[1])
                y.append(load_lrfu(file_name, 25000000) * 100)
                yerr.append(0.0)

        elif c[0] == 'V' and c[-2] == "R":
            for j, size in enumerate(sizes):
                file_name = "results_S2/log_lrfu" + "_{}lru_{}_{}.txt".format(dataset, size[0], size[1])
                y.append(load_lrfu(file_name, 25000000) * 100)
                yerr.append(0.0)

        elif c[0] == 'P' and len(c) < 4: # PPD
            for j, size in enumerate(sizes):
                file_name = "results_S2/log_lrfu" + "_{}aifo_{}_{}.txt".format(dataset, size[0], size[1])
                y.append(load_lrfu(file_name, 25000000) * 100)
                yerr.append(0.0)
        
        elif c[0] == 'P' and len(c) < 7:
            file_name = "results_S2/log_p4lru_{}.txt".format(dataset)
            y = load_qmax(file_name, 25000000)
            for j in range(len(y)):
                y[j] *= 100

            yerr = [0 for j in range(len(x_coord))]
        
        else:
            for size in sizes:
                file_name = "results_S2//log_" + file_name_middle[i] + "_{}_{}_{}.txt".format(dataset, size[0], size[1])
                y.append(load_squid(file_name, 25000000, 0) * 100)
                yerr.append(0.0)
            

        print(y)

        ax.bar([x + w for x in x_coord], y[:len(x_coord)], yerr=yerr, error_kw=dict(ecolor='r', lw=3, capsize=7, capthick=1), width=0.33, alpha=1, edgecolor='k', linewidth=1, label=c, **styles[i])

        if i == 2:
            w += 0.29 * 1.1
        else:
            w += 0.42 * 1.1
    
    ax.set_xticks([1.5, 5, 8.5, 12, 15.5, 19])
    ax.set_yticks([10,20,30,40,50, 60,70,80,90,100])
    ax.set_xticklabels(sizes_labels, fontsize=37)
    ax.tick_params(axis='y', which='major', labelsize=40)
    ax.set_ylim(0, 100)
    ax.set_xlim(0, 21)

    # ax.text(0.5, -0.17, '({}) Zipf 0.{}'.format(['a', 'b'][nfig], dataset), horizontalalignment='center', verticalalignment='center', transform=ax.transAxes, fontdict={"fontsize": 20})


    

# fig.legend(exp_names, prop={'size': 24}, loc='lower center', framealpha=1.0, bbox_to_anchor=(0.5, -0.25), ncol=4)

# fig.tight_layout()
#plt.show()
# plt.tight_layout()
plt.savefig('figures/graph_lrfu_cache_{}.png'.format(dataset), bbox_inches='tight')
plt.savefig('figures/graph_lrfu_cache_{}.pdf'.format(dataset), bbox_inches='tight')
