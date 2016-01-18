#!/bin/sh

# Environment:
# S3_ACCESS_KEY_ID - must be set to S3 Access Key ID
# S3_SECRET_ACCESS_KEY - must be set to S3 Secret Access Key
# TEST_BUCKET_PREFIX - must be set to the test bucket prefix to use
# S3_COMMAND - may be set to s3 command to use, examples:
#              "valgrind s3"
#              "s3 -h" (for aws s3)
#              default: "s3"

total_tests=31
testnum=0
failed=0

if [ -z "$S3_ACCESS_KEY_ID" ]; then
    echo "S3_ACCESS_KEY_ID required"
    exit 77
fi

if [ -z "$S3_SECRET_ACCESS_KEY" ]; then
    echo "S3_SECRET_ACCESS_KEY required"
    exit 77
fi

if [ -z "$TEST_BUCKET_PREFIX" ]; then
    echo "TEST_BUCKET_PREFIX required"
    exit 77
fi

if [ -z "$S3_COMMAND" ]; then
    S3_COMMAND=s3
fi

TEST_BUCKET=${TEST_BUCKET_PREFIX}.testbucket

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

ok "Create the test bucket" "$S3_COMMAND create $TEST_BUCKET"

ok "List to find it" "$S3_COMMAND list | grep '^$TEST_BUCKET '"

ok "Test it" "$S3_COMMAND test $TEST_BUCKET"
ok "Test it with certificate validation" "$S3_COMMAND -v test $TEST_BUCKET"
ok "Test it with vhost-style URLs" "$S3_COMMAND -h test $TEST_BUCKET"
ok "Test it with vhost-style URLs and certificate verification" "$S3_COMMAND -h -v test $TEST_BUCKET"

ok "List to ensure that it is empty" "
    $S3_COMMAND list $TEST_BUCKET | awk 'NR==3 {exit 1}'
"

ok "Put some data" "
    rm -f seqdata;
    seq 1 10000 > seqdata;
    $S3_COMMAND put $TEST_BUCKET/testkey filename=seqdata noStatus=1
"

ok "Get the data and make sure that it matches" "
    rm -f testkey;
    $S3_COMMAND get $TEST_BUCKET/testkey filename=testkey;
    $S3_COMMAND get $TEST_BUCKET/testkey filename=testkey;
    diff seqdata testkey;
    myres=$?;
    rm -f seqdata testkey;
    return $myres;
"

ok "Delete the file" "$S3_COMMAND delete $TEST_BUCKET/testkey"

ok "Remove the test bucket" "$S3_COMMAND delete $TEST_BUCKET"

ok "Make sure it's not there" "$S3_COMMAND list | grep '^$TEST_BUCKET '" 1

ok "Now create it again" "$S3_COMMAND create $TEST_BUCKET"
ok "Put 10 files in it" "
    myfail=0;
    for i in $(seq 0 9); do
        echo Hello | $S3_COMMAND put $TEST_BUCKET/key_\$i || myfail=\$((\$myfail + 1));
    done;
    return \$myfail;
"

ok "List with all details" "
    $S3_COMMAND list $TEST_BUCKET |
    awk 'NR > 2 {
        key=\$1; size=\$3;
        if (size != 6) {
            exit 1
        }
        if (match(key, /^key_[0-9]\$/) == 0) {
            exit 1
        }
    }'
"

COPY_BUCKET="${TEST_BUCKET_PREFIX}.copybucket"

ok "Create another test bucket copy a file into it" "
    $S3_COMMAND create $COPY_BUCKET;
    $S3_COMMAND copy $TEST_BUCKET/key_5 $COPY_BUCKET/copykey
"

ok "List the copy bucket" "$S3_COMMAND list $COPY_BUCKET | 
    awk 'NR > 2 {
        key=\$1;
        if (match(key, /^copykey\$/) == 0) {
            exit 1
        }
    }'
"

ok "Compare the files" "
    myfail=0;
    rm -f key_5 copykey;
    $S3_COMMAND get $TEST_BUCKET/key_5 filename=key_5
        || myfail=\$((\$myfail + 1));
    $S3_COMMAND get $COPY_BUCKET/copykey filename=copykey
        || myfail=\$((\$myfail + 1));
    diff key_5 copykey
        || myfail=\$((\$myfail + 1));
    rm -f key_5 copykey;
    return \$myfail
"

ok "Delete the files" "
    myfail=0;
    for i in $(seq 0 9); do
        $S3_COMMAND delete $TEST_BUCKET/key_\$i
        || myfail=\$((\$myfail+1));
    done;
    $S3_COMMAND delete $COPY_BUCKET/copykey
        || myfail=\$((\$myfail+1));
    return \$myfail
"

ok "Delete the copy bucket" "$S3_COMMAND delete $COPY_BUCKET"

ok "Now create a new zero-length file" "$S3_COMMAND put $TEST_BUCKET/aclkey < /dev/null"

ok "Get the bucket acl" "
    rm -f acl;
    $S3_COMMAND getacl $TEST_BUCKET filename=acl"

ACL_FORMAT="%-6s  %-90s  %-12s\\n"

ok "Add READ for all AWS users, and READ_ACP for everyone" "
    echo 'Group Authenticated AWS Users READ' >> acl;
    echo 'Group All Users READ_ACP' >> acl;
    $S3_COMMAND setacl $TEST_BUCKET filename=acl
"

ok "Test to make sure that it worked" "
    myfail=0;
    rm -f acl_new;
    $S3_COMMAND getacl $TEST_BUCKET filename=acl_new
        || myfail=\$((\$myfail + 1));
    diff -b acl acl_new 
        || myfail=\$((\$myfail + 1));
    rm -f acl acl_new;
    return \$myfail
"

ok "Get the key acl" "
    rm -f acl;
    $S3_COMMAND getacl $TEST_BUCKET/aclkey filename=acl
"

ok "Add READ for all AWS users, and READ_ACP for everyone" "
    printf '$ACL_FORMAT' \
        'Group' 'Authenticated AWS Users' 'READ' \
        'Group' 'All Users' 'READ_ACP' >> acl;
    $S3_COMMAND setacl $TEST_BUCKET/aclkey filename=acl
"

ok "Test to make sure that it worked" "
    myfail=0;
    rm -f acl_new;
    $S3_COMMAND getacl $TEST_BUCKET/aclkey filename=acl_new
        || myfail=\$((\$myfail + 1));
    diff -b acl acl_new
        || myfail=\$((\$myfail + 1));
    rm -f acl acl_new;
    return \$myfail;
"

ok "Delete the acl key" "$S3_COMMAND delete $TEST_BUCKET/aclkey"

ok "Check multipart file upload (>15MB)" "
    myfail=0;
    dd if=/dev/zero of=mpfile bs=1024k count=30;
    $S3_COMMAND put $TEST_BUCKET/mpfile filename=mpfile
        || myfail=\$((\$myfail + 1));
    $S3_COMMAND get $TEST_BUCKET/mpfile filename=mpfile.get
        || myfail=\$((\$myfail + 1));
    diff mpfile mpfile.get
        || myfail=\$((\$myfail + 1));
    rm -f mpfile mpfile.get
"

ok "Remove the test file" "$S3_COMMAND delete $TEST_BUCKET/mpfile"

ok "Remove the test bucket" "$S3_COMMAND delete $TEST_BUCKET"

exit $failed
