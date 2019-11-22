def generate_script(prefix, s_cnt, s_timeout, c_cnt, c_timeout, req_intvl, payload_sz):
    script = []

    fn_prefix = '{}_{}_{}_{}_{}_{:03d}_{:05d}'.format(prefix, s_cnt, s_timeout, c_cnt, c_timeout, req_intvl, payload_sz)

    log_files = []
    for i in range(1, s_cnt + 1):
        log_fn = '{}_s{}.log'.format(fn_prefix, i)
        log_files.append(log_fn)
        script.append('timeout {} ./App_raft {} | tee {} || date -Ins&'.format(s_timeout, i, log_fn))

    script.append('sleep 2s')

    for i in range(1, c_cnt + 1):
        script.append('sleep 1s')
        log_fn = '{}_c{}.log'.format(fn_prefix, i)
        log_files.append(log_fn)
        cmd = 'timeout {} ./client {} {} {} | tee {} || date -Ins&'.format(c_timeout, i, req_intvl, payload_sz, log_fn)
        script.append(cmd)

    script.append('sleep {}s'.format(30 - int(c_cnt)))  # int(s_timeout.rstrip('s')) - 2 - int(c_cnt))

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

    # # number of nodes
    # for server_cnt in range(2, 9):
    #     s = generate_script('ER', server_cnt, '25s', 1, '12s', 10, 32)
    #     test_cases.append(s)

    # request frequency
    # for request_interval in [100, 80, 60, 50, 40, 30, 20, 15, 10, 8, 5, 4, 3, 2, 1]:
    #     s = generate_script('ER', 5, '25s', 1, '15s', request_interval, 32)
    #     test_cases.append(s)

    for client_cnt in range(5,10):
        s = generate_script('ER', 5, '25s', client_cnt, '15s', 1, 32)
        test_cases.append(s)

    # # request frequency
    # for request_interval in [100, 80, 60, 50, 40, 30, 20, 15, 10, 8, 5, 4, 3, 2, 1]:
    #     s = generate_script('ER', 3, '25s', 1, '15s', request_interval, 32)
    #     test_cases.append(s)

    for client_cnt in range(5,10):
        s = generate_script('ER', 3, '25s', client_cnt, '15s', 1, 32)
        test_cases.append(s)

    # # payload size
    # for payload_term in range(4, 17):
    #     s = generate_script('ER', 3, '25s', 1, '12s', 10, 2 ** payload_term)
    #     test_cases.append(s)

    # # payload size
    # for payload_term in range(4, 17):
    #     s = generate_script('ER', 5, '25s', 1, '12s', 10, 2 ** payload_term)
    #     test_cases.append(s)



    print('\n'.join(test_cases))
