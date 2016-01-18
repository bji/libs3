#!/bin/sh

# Environment:

total_tests=28
testnum=0
failed=0

echo "1..$total_tests"

myexec()
{
    eval "$@"
}

ok()
{
    name="$1"
    cmd="$2"
    expected_result="${3:-0}"
    testnum=$(($testnum + 1))

    resultstring="$(myexec $cmd)"

    rc=$?
    
    if [ $rc -eq $expected_result ]; then
        echo "ok $testnum - $name"
        echo "# $(echo $cmd | tr -d '\n')"
        rc=0

    else
        echo "not ok $testnum - $name"
        failed=$(($failed + 1))
        rc=1
    fi
    echo "$resultstring" | while read line; do
        echo "#     $line"
    done
    return $rc
}

ok "Test XML with entity references and CDATA" "testsimplexml < ${top_srcdir}/test/goodxml_01.xml"

ok "Test XML with deeply nested elements" "testsimplexml < ${top_srcdir}/test/goodxml_02.xml"

ok "Test XML with large text data" "testsimplexml < ${top_srcdir}/test/goodxml_03.xml"

ok "Test XML with overlong element path" "testsimplexml < ${top_srcdir}/test/badxml_01.xml" 255

exit $failed
