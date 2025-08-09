build-profile:
	~/ysda/ydb/ya make --build=profile -k --no-bazel-remote-store -vvv
	cp ./arun ./profile.arun
build-debug:
	~/ysda/ydb/ya make --sanitize=undefined -k --no-bazel-remote-store --build=debug
	cp ./arun ./debug.arun

magic-trace:build-profile
	magic-trace run -snapshot-size 1M -trigger symbol:MagicTraceTrigger -- ./profile.arun -r 30000000 -a ext-me-simd

perf-simd:build-profile
	ALG_KIND=ext-me-simd ROWS=30000000 BUFFSIZE=48 KEYCOUNT=3 UNIQKEYS=100 bash profile.sh
# 	perf record -o ./perf.simd.hotspot-like.data -F 10000 --call-graph fp --aio -z --sample-cpu ./profile.arun -i /home/nfrmtk/ysda/ydb/campus/hashing-sorting/agen/akeys1Tx8x100.dat -a ext-me-simd -r 10000000 --part-2-buffer-size 48
# 	perf script -i ./perf.simd.hotspot-like.data > ./profile.simd.linux-perf.txt

perf-linear:build-profile
	ALG_KIND=ext-me ROWS=30000000 BUFFSIZE=32 KEYCOUNT=3 UNIQKEYS=100 bash profile.sh
# 	perf record -o ./perf.linear.hotspot-like.data -F 10000 --call-graph fp --aio -z --sample-cpu ./profile.arun -i /home/nfrmtk/ysda/ydb/campus/hashing-sorting/agen/akeys1Tx8x100.dat -a ext-me -r 10000000
# 	perf script -i ./perf.linear.hotspot-like.data > ./profile.linear.linux-perf.txt

