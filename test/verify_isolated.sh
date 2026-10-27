#!/usr/bin/env bash
# --isolated の入力検査（source/rule_set_verify.hpp）のテスト。
#
# 使い方: test/verify_isolated.sh [重なりのない rule file] [生の FIB]
#   重なりのない rule file: 通るべき 1 field の LPM rule（例: fib_to_flowbench.py の出力）
#   生の FIB:               失敗するべき FIB（1 行 "<ip> <len> <nexthop>"）。
#                           default route を除いて "@ip/len" に変換して渡す
# どちらも省略できる。小さい内蔵ケースは常に実行する。

set -u
cd "$(dirname "$0")/.."

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
BIN="$WORK/flowbench-trace"
g++ -std=c++17 -O2 source/trace_generator.cpp -o "$BIN" || exit 1

FAILED=0

# expect <名前> <期待する終了コード> <stdout+stderr に含まれるべき文字列。無ければ "-"> <入力 file> <flowbench-trace の引数...>
expect() {
    local name=$1 code=$2 text=$3 input=$4
    shift 4
    local log="$WORK/$name.log" start end
    start=$(date +%s.%N)
    "$BIN" -i "$input" -o "$WORK/$name.trace" "$@" >"$log" 2>&1
    local rc=$?
    end=$(date +%s.%N)
    local ok=1
    [ "$rc" -eq "$code" ] || ok=0
    if [ "$text" != "-" ] && ! grep -qF -- "$text" "$log"; then
        ok=0
    fi
    if [ "$ok" -eq 1 ]; then
        printf 'PASS %-28s rc=%d %.2fs\n' "$name" "$rc" "$(echo "$end - $start" | bc)"
    else
        printf 'FAIL %-28s rc=%d (expected %d, text "%s")\n' "$name" "$rc" "$code" "$text"
        sed 's/^/    /' "$log" | tail -5
        FAILED=1
    fi
}

LPM1=(-f 1 -fw 32 -ft LPM -n 100 -rd 1 1 -fd 1 1 -s 1)
LPM2=(-f 2 -fw 32 32 -ft LPM LPM -n 100 -rd 1 1 -fd 1 1 -s 1)

printf '@1.0.0.0/24\n@1.0.1.0/24\n@2.0.0.0/8\n@1.0.2.0/23\n' >"$WORK/disjoint1.txt"
printf '@1.0.0.0/24\n@2.0.0.0/8\n@1.0.0.0/16\n' >"$WORK/nested1.txt"
printf '@1.0.0.0/24\n@3.0.0.0/8\n@1.0.0.0/24\n' >"$WORK/duplicate1.txt"
printf '@1.0.0.0/32\n@1.0.0.1/32\n@1.0.0.0/31\n' >"$WORK/host1.txt"
# host route の後ろに大きいアドレス: /32 の getMax() が誤っていると失敗する
printf '@1.0.0.1/32\n@1.0.1.0/24\n@9.0.0.0/8\n' >"$WORK/host-disjoint1.txt"
# rule 1 と 2 は field 0 だけ重なり、rule 1 と 3 は両方の field で重なる:
# 重なる組は field 0 でソートしたとき隣にならない
printf '@1.0.0.0/8 10.0.0.0/8\n@1.0.0.0/16 20.0.0.0/8\n@1.1.0.0/16 10.1.0.0/16\n' >"$WORK/overlap2.txt"
printf '@1.0.0.0/8 10.0.0.0/8\n@1.0.0.0/16 20.0.0.0/8\n@1.1.0.0/16 30.0.0.0/8\n' >"$WORK/disjoint2.txt"

expect 1field-disjoint       0 "Isolated input verified: 4 rules" "$WORK/disjoint1.txt"   "${LPM1[@]}" --isolated
expect 1field-nested         1 "input rules 1 and 3 overlap"      "$WORK/nested1.txt"     "${LPM1[@]}" --isolated
expect 1field-duplicate      1 "input rules 1 and 3 overlap"      "$WORK/duplicate1.txt"  "${LPM1[@]}" --isolated
expect 1field-host-route     1 "overlap"                          "$WORK/host1.txt"       "${LPM1[@]}" --isolated
expect 1field-host-disjoint  0 "Isolated input verified: 3 rules" "$WORK/host-disjoint1.txt" "${LPM1[@]}" --isolated
expect 2field-overlap        1 "input rules 1 and 3 overlap"      "$WORK/overlap2.txt"    "${LPM2[@]}" --isolated
expect 2field-disjoint       0 "Isolated input verified: 3 rules" "$WORK/disjoint2.txt"   "${LPM2[@]}" --isolated
expect no-verify             0 -                                  "$WORK/nested1.txt"     "${LPM1[@]}" --isolated --no-verify
if grep -qF "Isolated input verified" "$WORK/no-verify.log"; then
    echo "FAIL no-verify printed the verification line"
    FAILED=1
fi

if [ $# -ge 1 ]; then
    expect fib-disjoint 0 "Isolated input verified:" "$1" "${LPM1[@]}" --isolated
    grep -F "Isolated input verified" "$WORK/fib-disjoint.log"
fi
if [ $# -ge 2 ]; then
    awk 'NF >= 2 && $2 != 0 { print "@" $1 "/" $2 }' "$2" >"$WORK/raw_fib.txt"
    expect fib-overlapping 1 "overlap" "$WORK/raw_fib.txt" "${LPM1[@]}" --isolated
    grep -A2 -F -- "--isolated:" "$WORK/fib-overlapping.log"
fi

if [ "$FAILED" -eq 0 ]; then
    echo "all tests passed"
fi
exit "$FAILED"
