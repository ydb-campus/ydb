mkdir -p profdata.d

alg=$ALG_KIND
rows=$ROWS
buffsize=$BUFFSIZE
keycount=$KEYCOUNT
keycardinality=$UNIQKEYS
if [[ -v BUFFSIZE ]] || [[ -v UNIQKEYS ]]; then
  echo "variables defined"
else
  echo "BUFFSIZE is not declared. declare all variables and try again"
  exit 1
fi



#perf record -o ./profdata.d/perf.$alg.hotspot-like.data -F 10000 --call-graph fp --aio -z --sample-cpu ./profile.arun -i /home/nfrmtk/ysda/ydb/campus/hashing-sorting/agen/akeys1Tx8x100.dat -a $alg -r $rows --part-2-buffer-size $buffsize -k $keycount -c $keycardinality
#perf script -i ./profdata.d/perf.$alg.hotspot-like.data > ./profdata.d/profile.$alg.rows-$rows.buffsize-$buffsize.keycnt-$keycount.keycard-$keycardinality.linux-perf.txt
