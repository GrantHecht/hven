#!/usr/bin/env bash
# cost.sh LABEL [BASE] -- minors/facts totals over cells kOptimal on both
O=$(dirname $0)/out; b=${2:-base}
join <(awk '{print $1,$2,$3,$4}' $O/$b.corpus) <(awk '{print $1,$2,$3,$4}' $O/$1.corpus) | awk '$2=="status=0" && $5=="status=0" {split($3,a,"=");split($4,c,"=");split($6,d,"=");split($7,e,"=");ma+=a[2];fa+=c[2];mb+=d[2];fb+=e[2];n++; if(a[2]!=d[2])mv++} END{printf "both-optimal %d: minors %d -> %d (%+.2f%%), facts %d -> %d (%+.2f%%), moved %d\n",n,ma,mb,100*(mb-ma)/ma,fa,fb,100*(fb-fa)/fa,mv}'
