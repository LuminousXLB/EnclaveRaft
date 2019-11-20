def generate_script_reqfreq(prefix, s_cnt, s_timeout, c_cnt, c_timeout, request_interval):
    script = []

    fn_prefix = '{}_{}_{}_{}_{}_{}'.format(prefix, s_cnt, s_timeout, c_cnt, c_timeout, request_interval)
    log_files = []
    for i in range(1, s_cnt + 1):
        log_fn = '{}_s{}.log'.format(fn_prefix, i)
        log_files.append(log_fn)
        script.append('timeout {} ./App_raft {} | tee {} || date -Ins&'.format(s_timeout, i, log_fn))

    script.append('sleep 1s')

    for i in range(1, c_cnt + 1):
        script.append('sleep 1s')
        log_fn = '{}_c{}.log'.format(fn_prefix, i)
        log_files.append(log_fn)
        script.append(
            'timeout {} ./client {} {} 256 | tee {} || date -Ins&'.format(c_timeout, i, request_interval, log_fn))

    script.append('sleep {}s'.format(int(s_timeout.rstrip('s')) - int(c_cnt)))

    fragmentation = ' '.join(log_files)
    package = '../../log_parser/{}'.format(fn_prefix)
    script += [
        'cat {} > {}.log'.format(fragmentation, package),
        'tar -cf {}.tar {}'.format(package, fragmentation),
        'rm {}'.format(fragmentation),
    ]

    return '\n'.join(script) + '\n\n'


def generate_script_payloadsz(prefix, s_cnt, s_timeout, c_cnt, c_timeout, payload_size):
    script = []

    fn_prefix = '{}_{}_{}_{}_{}_{}'.format(prefix, s_cnt, s_timeout, c_cnt, c_timeout, payload_size)
    log_files = []
    for i in range(1, s_cnt + 1):
        log_fn = '{}_s{}.log'.format(fn_prefix, i)
        log_files.append(log_fn)
        script.append('timeout {} ./App_raft {} | tee {} || date -Ins&'.format(s_timeout, i, log_fn))

    script.append('sleep 1s')

    for i in range(1, c_cnt + 1):
        script.append('sleep 1s')
        log_fn = '{}_c{}.log'.format(fn_prefix, i)
        log_files.append(log_fn)
        script.append('timeout {} ./client {} 1 {} | tee {} || date -Ins&'.format(c_timeout, i, payload_size, log_fn))

    script.append('sleep {}s'.format(int(s_timeout.rstrip('s')) - int(c_cnt)))

    fragmentation = ' '.join(log_files)
    package = '../../log_parser/{}'.format(fn_prefix)
    script += [
        'cat {} > {}.log'.format(fragmentation, package),
        'tar -cf {}.tar {}'.format(package, fragmentation),
        'rm {}'.format(fragmentation),
    ]

    return '\n'.join(script) + '\n\n'


if __name__ == '__main__':
    test_cases = []

    # for size in range(3, 17):
    #     s = generate_script_payloadsz('ER', 5, '20s', 1, '10s', 2 ** size)
    #     test_cases.append(s)

    for size in [1024, 8192, 16384, 32768]:
        s = generate_script_payloadsz('ER', 5, '20s', 1, '10s', size)
        test_cases.append(s)

    print('\n'.join(test_cases))
