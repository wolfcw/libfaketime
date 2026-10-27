#
# Checks that the --freeze wrapper option stops the clock at the given
# timestamp instead of letting it run.
#
# The default wrapper behavior resolves the timestamp through the date(1)
# helper and applies the result as an offset, so the clock keeps running.
# --freeze asks the same helper for an absolute timestamp instead, which
# libfaketime reports as a stopped clock. Because the helper is still used,
# --freeze also accepts the "date -d" syntax that cannot be passed to
# libfaketime directly.
#
# --freeze is a wrapper option only and cannot be combined with -f, which
# stops the clock by itself. It is therefore not exercised through fakecmd(),
# which sets FAKETIME directly and bypasses the wrapper.

init()
{
    typeset testsuite="$1"
    PLATFORM=$(platform)
    if [ -z "$PLATFORM" ]; then
        echo "$testsuite: unknown platform! quitting"
        return 1
    fi

    if [ "$PLATFORM" = "mac" ]; then
        DATE_PROG=gdate
    else
        DATE_PROG=date
    fi

    # --freeze resolves timestamps through the same date(1) helper as the
    # default behavior, so it is only meaningful where that helper works.
    if ! ${DATE_PROG} -d "2014-07-21 09:00:00" +%s >/dev/null 2>&1; then
        echo "$testsuite: no GNU-compatible date(1)! skipping"
        WRAPPER_OK=0
        return 0
    fi

    FT="${FAKETIME_TESTBIN:-../src/faketime}"
    if [ ! -x "$FT" ] && ! command -v "$FT" >/dev/null 2>&1; then
        echo "$testsuite: wrapper $FT not found! skipping"
        WRAPPER_OK=0
        return 0
    fi

    # The wrapper preloads the library from its compile-time PREFIX. In a
    # non-installed build that path usually does not exist, and the loader
    # just warns and continues unfaked, which would make every assertion
    # below meaningless. Presetting LD_PRELOAD to the in-tree library lets
    # the wrapper append its own (missing) entry harmlessly.
    WRAPPER_PRELOAD=
    if [ "$($FT "2014-07-21 09:00:00" date +%Y 2>/dev/null)" != "2014" ]; then
        if [ -f ../src/libfaketime.so.1 ]; then
            WRAPPER_PRELOAD=$(cd ../src && pwd)/libfaketime.so.1
        fi
        if [ "$(LD_PRELOAD="$WRAPPER_PRELOAD" $FT "2014-07-21 09:00:00" date +%Y 2>/dev/null)" != "2014" ]; then
            echo "$testsuite: wrapper cannot load libfaketime! skipping"
            WRAPPER_OK=0
            return 0
        fi
        echo "# using LD_PRELOAD=$WRAPPER_PRELOAD"
    fi

    WRAPPER_OK=1
    echo "# PLATFORM=$PLATFORM"
    return 0
}

run()
{
    init
    if [ "$WRAPPER_OK" -ne 1 ]; then
        echo "# skipped: wrapper not usable in this environment"
        return 0
    fi

    run_testcase freeze_holds
    run_testcase default_keeps_running
    run_testcase freeze_accepts_date_syntax
    run_testcase freeze_rejects_advanced_format
}

get_token()
{
    string=$1
    token_index=$2
    separator=$3

    echo $string | cut -d "$separator" -f $token_index
}

assert_timestamps_neq()
{
    timestamps=$1
    msg=$2

    first_timestamp=$(get_token "${timestamps}" 1 ' ')
    second_timestamp=$(get_token "${timestamps}" 2 ' ')

    assertneq "${first_timestamp}" "${second_timestamp}" "${msg}"
}

assert_timestamps_eq()
{
    timestamps=$1
    msg=$2

    first_timestamp=$(get_token "${timestamps}" 1 ' ')
    second_timestamp=$(get_token "${timestamps}" 2 ' ')

    asserteq "${first_timestamp}" "${second_timestamp}" "${msg}"
}

get_timestamps()
{
    wrapper_flags=$1
    shift

    # ${wrapper_flags} is intentionally unquoted: empty for the default
    # behavior, --freeze when freezing.
    LD_PRELOAD="$WRAPPER_PRELOAD" $FT ${wrapper_flags} "2014-07-21 09:00:00" \
    bash -c "date +%s; sleep 2; date +%s"
}

freeze_holds()
{
    expected=$(${DATE_PROG} -d "2014-07-21 09:00:00" +%s)
    timestamps=$(get_timestamps --freeze)
    msg="When freezing, timestamps should be equal"
    assert_timestamps_eq "${timestamps}" "${msg}"
    asserteq "$(get_token "${timestamps}" 1 ' ')" "${expected}" \
        "Frozen timestamp should be the requested one"
}

default_keeps_running()
{
    timestamps=$(get_timestamps "")
    msg="Without --freeze, timestamps should be different"
    assert_timestamps_neq "${timestamps}" "${msg}"
}

freeze_accepts_date_syntax()
{
    timestamp=$(LD_PRELOAD="$WRAPPER_PRELOAD" $FT --freeze "last friday 5 pm" date +%Y)
    expected=$(${DATE_PROG} -d "last friday 5 pm" +%Y)
    msg="--freeze should accept date(1) syntax"
    asserteq "${timestamp}" "${expected}" "${msg}"
}

freeze_rejects_advanced_format()
{
    LD_PRELOAD="$WRAPPER_PRELOAD" $FT -f --freeze "2014-07-21 09:00:00" true >/dev/null 2>&1
    retval=$?
    msg="--freeze should be rejected together with -f"
    assertneq "${retval}" "0" "${msg}"
}
