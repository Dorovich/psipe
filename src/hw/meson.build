psipe_ss = ss.source_set()
psipe_ss.add(files(
    'dma.c',
    'irq.c',
    'mmio.c',
    'proxy.c',
    'psipe.c',
))

system_ss.add_all(when: 'CONFIG_PSIPE', if_true: psipe_ss)
