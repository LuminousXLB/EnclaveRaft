def generate_script(prefix, s_cnt, s_timeout, c_cnt, c_timeout, request_interval):
    header = 'basic_test_case() {'
    tail = '}'

    script = []

    fn_prefix = '{}_{}_{}_{}_{}_{}'.format(prefix, s_cnt, s_timeout, c_cnt, c_timeout, request_interval)
    log_files = []
    for i in range(1, s_cnt + 1):
        log_fn = '{}_s{}.log'.format(fn_prefix, i)
        log_files.append(log_fn)
        script.append('timeout {} ./App_raft {} | tee {} || date -Ins&'.format(s_timeout, i, log_fn))

    script.append('sleep 2s')

    for i in range(1, c_cnt + 1):
        log_fn = '{}_c{}.log'.format(fn_prefix, i)
        log_files.append(log_fn)
        script.append('timeout {} ./client {} | tee {} || date -Ins&'.format(c_timeout, i, log_fn))

    script.append('sleep {}'.format(s_timeout))

    fragmentation = ' '.join(log_files)
    package = '../../log_parser/{}'.format(fn_prefix)
    script += [
        'cat {} > {}.log'.format(fragmentation, package),
        'tar -cf {}.tar {}'.format(package, fragmentation),
        'rm {}'.format(fragmentation),
    ]

    output = [header]
    for line in script:
        output.append('  ' + line)
    output.append(tail)

    return '\n'.join(output)


if __name__ == '__main__':
    s = generate_script('ER', 5, '15s', 1, '10s', 50)
    print(s)
